#include "new_hash.h"
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
    memset(new_Segment, 0, sizeof(struct Segment));

    new_Segment->pattern = ((seg->pattern) << 1) + 1;
    new_Segment->local_depth = seg->local_depth + 1;
    u64 cmp_pattern = new_Segment->pattern << (64 - new_Segment->local_depth);
    #ifdef DEBUG_ERROR
        printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);
    #endif
    u64 i;
    for (i = 0; i < kNumSlot/kNumPair; ++i)
    {
        u64 start_index = i * kNumPair;
        u64 list = seg->_[start_index].key;
        if(list == 0){
            //该组bucket未初始化
            #ifdef DEBUG_ERROR
                printf("i = %d, list = 0\n", i);
            #endif
            continue;
        }
        u64 num = seg->_[start_index].value;

        //找到分裂的位置,即sort_index
        s64 left = 0, right = num - 1, mid = 0;
        s64 cmp_index = 0, sort_index = 0;
        u64 slot_key = 0, slot_hash = 0, slot = 0;
        while(left < right) {
            mid = (right + left) / 2;
            cmp_index = (list >> (60 - mid * 4)) & 15;
            slot = start_index + cmp_index;
            slot_key = seg->_[slot].key;
            slot_hash = hash_64(slot_key);
            if (slot_hash == cmp_pattern) {
                s64 k = mid - 1;
                while(k>=0){
                    cmp_index = (list >> (60 - k * 4)) & 15;
                    slot = start_index + cmp_index;
                    slot_key = seg->_[slot].key;
                    slot_hash = hash_64(slot_key);
                    --k;
                    if(slot_hash != cmp_pattern)
                        break;
                }
                sort_index = k + 1;
                break;
            } else if (slot_hash < cmp_pattern) {
                left = mid + 1;
            } else if (slot_hash > cmp_pattern) {
                right = mid;
            }
        }
        if(left > right){
            //即当num == 0时的情况
            continue;
        }
        else if(left == right){
            cmp_index = (list >> (60 - left * 4)) & 15;
            slot = start_index + cmp_index;
            slot_key = seg->_[slot].key;
            slot_hash = hash_64(slot_key);
            if (slot_hash < cmp_pattern) {
                continue;
            } else if (slot_hash >= cmp_pattern) {
                //left为插入的位置
                sort_index = left;
            }
        }

        #ifdef DEBUG_ERROR
            printf("i = %d, sort_index = %d\n", i, sort_index);
        #endif

        //将元素移动
        u64 j;
        u64 new_Segment_index = 15;
        u64 new_Segment_num = 0;
        u64 new_list = INIT_LIST;
        for(j = num - 1; j >= sort_index; j++){
            u64 new_Segment_slot = new_Segment_index + start_index;
            cmp_index = (list >> (60 - j * 4)) & 15;
            slot = start_index + cmp_index;
            new_Segment->_[new_Segment_slot].value = seg->_[slot].value;
            new_Segment->_[new_Segment_slot].key = seg->_[slot].key;
            pmem_persist(&seg->_[new_Segment_slot], sizeof(Pair));
            mfence();
            new_list = (new_list >> 4) + (new_Segment_index << 60);
            --new_Segment_index;
            ++new_Segment_num;
            #ifdef DEBUG_ERROR
                printf("i = %d, j = %d, new_list = %d\n", i, j, new_list);
            #endif
        }

        new_Segment->_[start_index].value = new_Segment_num;
        new_Segment->_[start_index].key = new_list;
        pmem_persist(&seg->_[start_index], sizeof(Pair));
        mfence();

        //根据sort_index更新0key
        seg->_[start_index].key = 
            ((list << (num * 4 - sort_index * 4)) & (0xFFFFFFFFFFFFFFFF >> (sort_index * 4))) 
            + ((list >> (64 - num * 4)) & (0xFFFFFFFFFFFFFFFF << (64 - num * 4 + sort_index * 4) >> (64 - num * 4 + sort_index * 4)))
            + (list & (0xFFFFFFFFFFFFFFFF << (60 - sort_index * 4) << 4));
        seg->_[start_index].value = num - new_Segment_num;
        pmem_persist(&seg->_[start_index], sizeof(Pair));
        mfence();
        #ifdef DEBUG_ERROR
            printf("i = %d, list = %x, num = %d\n", i, seg->_[start_index].key, seg->_[start_index].value);
        #endif
    }
    return new_Segment;
}

int insert_hash(HASH *dir, Key_t new_key, Value_t new_value)
{
    Key_t key_hash = hash_64(new_key);
    Key_t dir_index = key_hash >> (key_size - dir->global_depth);
    u64 start_index = (key_hash & kMask) * kNumPair;
    #ifdef DEBUG_ERROR
        printf("key = %016x, key_hash = %016llx, x = %016llx, y = %016llx global_depth = %d\n",new_key,key_hash,dir_index,start_index,dir->global_depth);
    #endif
    Segment *seg = dir->_->_[dir_index];
    u64 num = seg->_[start_index].value;
    if(num < 15){
        u64 list = seg->_[start_index].key;
        if(list == 0){
            //该组bucket未初始化
            list = INIT_LIST;
        }
        
        u64 insert_index =  list & 15;
        s64 slot = start_index + insert_index;
        //插入数据
        seg->_[slot].value = new_value;
        seg->_[slot].key = new_key;
        pmem_persist(&seg->_[slot], sizeof(Pair));
        mfence();
        
        //更新list和num
        s64 left = 0, right = num - 1, mid = 0;
        s64 cmp_index = 0, sort_index = 0;
        u64 slot_key = 0, slot_hash = 0;
        while(left < right) {
            mid = (right + left) / 2;
            cmp_index = (list >> (60 - mid * 4)) & 15;
            slot = start_index + cmp_index;
            slot_key = seg->_[slot].key;
            slot_hash = hash_64(slot_key);
            if (slot_hash == key_hash && slot_key == new_key) {
                return -1;
            } else if (slot_hash < key_hash || (slot_hash == key_hash && slot_key < new_key)) {
                left = mid + 1;
            } else if (slot_hash > key_hash || (slot_hash == key_hash && slot_key > new_key)) {
                right = mid;
            }
        }
        if(left > right){
            //即当num == 0时的情况
            sort_index = 0;
        }
        else if(left == right){
            cmp_index = (list >> (60 - left * 4)) & 15;
            slot = start_index + cmp_index;
            slot_key = seg->_[slot].key;
            slot_hash = hash_64(slot_key);
            if (slot_hash == key_hash && slot_key == new_key) {
                return -1;
            } else if (slot_hash < key_hash || (slot_hash == key_hash && slot_key < new_key)) {
                sort_index = left + 1;
            } else if (slot_hash > key_hash || (slot_hash == key_hash && slot_key > new_key)) {
                //left为插入的位置
                sort_index = left;
            }
        }
        //根据sort_index和insert_index更新0key
        seg->_[start_index].key = ((list >> 4) & (0xFFFFFFFFFFFFFFFF >> (sort_index * 4) >> 4)) 
                        + (insert_index << (60 - sort_index * 4))
                        + (list & (0xFFFFFFFFFFFFFFFF << (60 - sort_index * 4) << 4));
        seg->_[start_index].value = num + 1;
        pmem_persist(&seg->_[start_index], sizeof(Pair));
        mfence();
        return 1;
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
        u64 i;
        //Directory翻倍
        Directory *new_dir = getNode(HASH_DIRECTORY, 1<<new_Segment->local_depth);
        for (i = 0; i < (1 << (dir->global_depth)); ++i)
        {
            if (i == dir_index)
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
        u64 i;
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
        pmem_persist(&seg->local_depth, sizeof(size_t));
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
    Key_t dir_index = key_hash >> (key_size - dir->global_depth);
    u64 start_index = (key_hash & kMask) * kNumPair;

    Segment *seg = dir->_->_[dir_index];
    u64 num = seg->_[start_index].value;
    if(num == 0) return NONE;
    u64 list = seg->_[start_index].key;
    
    s64 left = 0, right = num - 1;

    while(left <= right) {
        s64 mid = (right + left) / 2;
        u64 cmp_index = (list >> (60 - mid * 4)) & 15;
        u64 slot = start_index + cmp_index;
        u64 slot_key = seg->_[slot].key;
        u64 slot_hash = hash_64(slot_key);

        if (slot_hash == key_hash && slot_key == search_key) {
            seg->_[start_index].key = ((list << 4) & (0xFFFFFFFFFFFFFFFF >> (mid * 4))) 
                            + cmp_index 
                            + (list & (0xFFFFFFFFFFFFFFFF << (60 - mid * 4) << 4));
            seg->_[start_index].value = num - 1;
            pmem_persist(&seg->_[start_index], sizeof(Pair));
            mfence();
        } else if (slot_hash < key_hash || (slot_hash == key_hash && slot_key < search_key)) {
            left = mid + 1;
        } else if (slot_hash > key_hash || (slot_hash == key_hash && slot_key > search_key)) {
            right = mid - 1;
        }
    }
    return NONE;
}

Value_t search_hash(HASH *dir, Key_t search_key)
{
    Key_t key_hash = hash_64(search_key);
    Key_t dir_index = key_hash >> (key_size - dir->global_depth);
    u64 start_index = (key_hash & kMask) * kNumPair;

    Segment *seg = dir->_->_[dir_index];
    u64 num = seg->_[start_index].value;
    if(num == 0) return NONE;
    u64 list = seg->_[start_index].key;

    s64 left = 0, right = num - 1;
    while(left <= right) {
        s64 mid = (right + left) / 2;
        u64 slot = start_index + ((list >> (60 - mid * 4)) & 15);
        u64 slot_key = seg->_[slot].key;
        u64 slot_hash = hash_64(slot_key);
        if (slot_hash == key_hash && slot_key == search_key) {
            return seg->_[slot].value;
        } else if (slot_hash < key_hash || (slot_hash == key_hash && slot_key < search_key)) {
            left = mid + 1;
        } else if (slot_hash > key_hash || (slot_hash == key_hash && slot_key > search_key)) {
            right = mid - 1;
        }
    }

    return NONE;
}

void init_hash(HASH *o_hash)
{
    o_hash->global_depth = 1;
    o_hash->_ = getNode(HASH_DIRECTORY, 2);
    Directory *init_dir = o_hash->_;
    
    init_dir->_[0] = getNode(HASH_SEGMENT, 0);
    memset(init_dir->_[0], 0, sizeof(Segment));
    init_dir->_[0]->local_depth = 1;
    init_dir->_[0]->pattern = 0;

    init_dir->_[1] = getNode(HASH_SEGMENT, 0);
    memset(init_dir->_[1], 0, sizeof(Segment));
    init_dir->_[1]->local_depth = 1;
    init_dir->_[1]->pattern = 1;
}
