#include "hash.h"
#include "memory_management.h"

#ifdef DEBUG_TIME
    u64 resize_time = 0;
    u64 resize_1_num = 0;
    u64 resize_2_num = 0;
    u64 malloc_time = 0;
    void print_resize()
    {
        printf("resize_time = %lld\n", resize_time);
        printf("resize_1_num = %lld\n", resize_1_num);
        printf("resize_2_num = %lld\n", resize_2_num);
    }
#endif

size_t unaligned_load(const char *p)
{
  size_t result;
  __builtin_memcpy(&result, p, sizeof(result));
  return result;
}

size_t load_bytes(const char *p, int n)
{
  size_t result = 0;
  --n;
  do
    result = (result << 8) + (unsigned char)(p[n]);
  while (--n >= 0);
  return result;
}

size_t shift_mix(size_t v)
{
  return v ^ (v >> 47);
}

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

Segment * Segment_Split(Segment *seg)
{
    Segment *new_Segment = getNode(HASH_SEGMENT, 0);
    memset(new_Segment, -1, sizeof(struct Segment));

    new_Segment->pattern = ((seg->pattern) << 1) + 1;
    new_Segment->local_depth = seg->local_depth + 1;
    #ifdef DEBUG_ERROR
        printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);
    #endif
    int i;
    for (i = 0; i < kNumSlot; ++i)
    {
        if (seg->_[i].key != INVALID)
        {
            Key_t re_hash_key = hash_64(seg->_[i].key);
            size_t pattern = re_hash_key >> (key_size - new_Segment->local_depth);
            if (pattern == new_Segment->pattern)
            {
                unsigned j;
                for (j = 0; j < kNumSlot; ++j)
                {
                    if (new_Segment->_[j].key == INVALID)
                    {
                        new_Segment->_[j].value = seg->_[i].value;
                        new_Segment->_[j].key = seg->_[i].key;
                        break;
                    }
                }
            }
        }
    }
    pmem_persist(new_Segment, sizeof(Segment));
    mfence();
    return new_Segment;
}

int insert_hash(HASH *dir, Key_t new_key, Value_t new_value)
{
    Key_t key_hash = hash_64(new_key);

    Key_t x = (key_hash >> (key_size - dir->global_depth));

    Segment *seg = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumSlot; ++i)
    {
        if (seg->_[i].key == INVALID ||
            (hash_64(seg->_[i].key) >> (key_size - seg->local_depth)) != seg->pattern)
        {
            seg->_[i].value = new_value;
            seg->_[i].key = new_key;
            pmem_persist(&seg->_[i], sizeof(Pair));
            mfence();
            //print(dir);
            return 1;
        }
    }

    if (seg->local_depth == dir->global_depth)
    {
        //分配一个新的Segment
        #ifdef DEBUG_TIME
            ++resize_1_num;
            struct timeval start, end;
            mfence();
            gettimeofday(&start, NULL);
            mfence();
        #endif
        #ifdef DEBUG_ERROR
            printf("split 1 begin...\n");
        #endif
        Segment *new_Segment = Segment_Split(seg);
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
        #ifdef DEBUG_TIME
            mfence();
            gettimeofday(&end, NULL);
            mfence();
            resize_time += (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        #endif
    }
    else
    {
        #ifdef DEBUG_TIME
            resize_2_num++;
            struct timeval start, end;
            mfence();
            gettimeofday(&start, NULL);
            mfence();
        #endif
        #ifdef DEBUG_ERROR
            printf("split 2 begin...\n");
        #endif
        /*for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        Segment *new_Segment = Segment_Split(seg);

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
        //printf("new_Segment %p\n", new_Segment);
        /*	for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        pmem_persist(seg, sizeof(Segment));
        mfence();
        #ifdef DEBUG_TIME
            mfence();
            gettimeofday(&end, NULL);
            mfence();
            resize_time += (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        #endif
    }
    return insert_hash(dir, new_key, new_value);
}

int delete_hash(HASH *dir, Key_t search_key)
{
    Key_t key_hash = hash_64(search_key);
    Key_t x = (key_hash >> (key_size - dir->global_depth));

    Segment *seg = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumSlot; ++i)
    {
        if (seg->_[i].key == search_key)
        {
            seg->_[i].key = INVALID;
            pmem_persist(&seg->_[i].key, sizeof(Key_t));
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

    Segment *seg = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumSlot; ++i)
    {
        if (seg->_[i].key == search_key)
        {
            return seg->_[i].value;
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
