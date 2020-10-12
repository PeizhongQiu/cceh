#include "hash.h"
#include "memory_management.h"


size_t _Hash_bytes(const void *ptr, size_t len, size_t seed)
{
  static const size_t mul = (((size_t)0xc6a4a793UL) << 32UL) + (size_t)0x5bd1e995UL;
  const char *const buf = (const char *)(ptr);

  // Remove the bytes not divisible by the sizeof(size_t).  This
  // allows the main loop to process the data as 64-bit integers.
  const size_t len_aligned = len & ~(size_t)0x7;
  const char *const end = buf + len_aligned;
  size_t hash = seed ^ (len * mul);
  const char *p = buf;
  for (; p != end; p += 8)
  {
    const size_t data = shift_mix(unaligned_load(p) * mul) * mul;
    hash ^= data;
    hash *= mul;
  }
  if ((len & 0x7) != 0)
  {
    const size_t data = load_bytes(end, len & 0x7);
    hash ^= data;
    hash *= mul;
  }
  hash = shift_mix(hash) * mul;
  hash = shift_mix(hash);
  return hash;
}

size_t hash_64(size_t val)
{
    return _Hash_bytes(&val, sizeof(size_t), 0xc70f6907UL);
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
            Segment *seg = dir->_->_[i];

            printf("(%016llx, %016llx)", seg->_[j].key, hash_64(seg->_[j].key));
        }
        printf("\n");
    }
}

int insert_hash(HASH *dir, Key_t new_key, Value_t new_value)
{
    Key_t key_hash = hash_64(new_key);

    Key_t x = (key_hash >> (key_size - dir->global_depth));
    u64 y = (key_hash & kMask) * kNumPairPerCacheLine * kNumCacheLine;
    //printf("key = %016x, key_hash = %016llx, x = %016llx, y = %016llx global_depth = %d\n",new_key,key_hash,x,y,dir->global_depth);
    Segment *seg = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u64 slot = (y + i) % kNumSlot;
        //printf("slot = %x slot.key = %016llx slot.hash_key = %016llx\n",slot,seg->_[slot].key, hash_64(seg->_[slot].key));
        if (seg->_[slot].key == INVALID ||
            (hash_64(seg->_[slot].key) >> (key_size - seg->local_depth)) != seg->pattern)
        {
            seg->_[slot].value = new_value;
            seg->_[slot].key = new_key;
            pmem_persist(&seg->_[slot], sizeof(Pair));
            mfence();
            //print(dir);
            return 1;
        }
    }

    if (seg->local_depth == dir->global_depth)
    {
        //分配一个新的Segment
        //printf("split 1 begin...\n");
        Segment *new_Segment = getNode(HASH_SEGMENT, 0);
        memset(new_Segment, -1, sizeof(struct Segment));

        new_Segment->pattern = ((seg->pattern) << 1) + 1;
        new_Segment->local_depth = seg->local_depth + 1;
        //printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);

        for (i = 0; i < kNumSlot; ++i)
        {
            if (seg->_[i].key != INVALID)
            {
                Key_t re_hash_key = hash_64(seg->_[i].key);
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
                            new_Segment->_[slot].value = seg->_[i].value;
                            new_Segment->_[slot].key = seg->_[i].key;
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
        Directory *new_dir = getNode(HASH_DIRECTORY, 1<<new_Segment->local_depth);
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

        ++seg->local_depth;
        pmem_persist(&seg->local_depth, sizeof(size_t));
        mfence();
        seg->pattern = seg->pattern << 1;
        pmem_persist(&seg->pattern, sizeof(size_t));
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
        Segment *new_Segment = getNode(HASH_SEGMENT, 0);
        /*for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        memset(new_Segment, -1, sizeof(struct Segment));
        new_Segment->pattern = ((seg->pattern) << 1) + 1;
        /*for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        new_Segment->local_depth = seg->local_depth + 1;
        //printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);

        for (i = 0; i < kNumSlot; ++i)
        {
            if (seg->_[i].key != INVALID)
            {
                Key_t re_hash_key = hash_64(seg->_[i].key);
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
                            new_Segment->_[slot].value = seg->_[i].value;
                            new_Segment->_[slot].key = seg->_[i].key;
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
        seg->pattern = seg->pattern << 1;
        pmem_persist(&seg->pattern, sizeof(size_t));
        mfence();
        ++seg->local_depth;
        //seg->pattern = seg->pattern << 1;
        //printf("new_Segment %p\n", new_Segment);
        /*	for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        //printf("insert_again\n");
        pmem_persist(seg, sizeof(Segment));
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

    Segment *seg = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u64 slot = (y + i) % kNumSlot;
        if (seg->_[slot].key == search_key)
        {
            seg->_[slot].key = INVALID;
            pmem_persist(&seg->_[slot].key, sizeof(Key_t));
            mfence();
            //            free(seg->_[slot].value);
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

    Segment *seg = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u64 slot = (y + i) % kNumSlot;
        if (seg->_[slot].key == search_key)
        {
            return seg->_[slot].value;
        }
    }
    return NONE;
}

// int update_hash(HASH *dir, Key_t search_key, Value_t new_value)
// {
//     u32 key_hash = hash_32(search_key, 32);
//     u32 x = (key_hash >> (key_size - dir->global_depth));
//     u32 y = (key_hash & kMask) * kNumPairPerCacheLine * kNumCacheLine;

//     Segment *seg = dir->_->_[x];
//     unsigned i;
//     for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
//     {
//         u32 slot = (y + i) % kNumSlot;
//         if (seg->_[slot].key == search_key)
//         {
//             seg->_[slot].value = new_value;
//             return 1;
//         }
//     }
//     return NONE;
// }

void init(HASH *o_hash)
{
    o_hash->global_depth = 1;
    o_hash->_ = getNode(HASH_DIRECTORY, 2);
    Directory *init_dir = o_hash->_;
    
    init_dir->_[0] = getNode(HASH_SEGMENT, 0);
    memset(init_dir->_[0], -1, sizeof(Segment));
    init_dir->_[0]->local_depth = 1;
    init_dir->_[0]->pattern = 0;

    init_dir->_[1] = getNode(HASH_SEGMENT, 0);
    memset(init_dir->_[1], -1, sizeof(Segment));
    init_dir->_[1]->local_depth = 1;
    init_dir->_[1]->pattern = 1;
}
