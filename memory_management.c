#include "memory_management.h"
#include <libpmem.h>
#include "hash.h"

//注意第二维度从1开始
//存放每个块的地址
void *table[HASH_TYPE][10000];
//NvmBlock * table[100000000/MEMORY_BLOCK_LEN +1];
//MemoryBlock * mem_table[100000000/MEMORY_BLOCK_LEN +1];

//每个nvmblock对应一个4M块
NVMBLOCK_DECLARE(Directory);
NVMBLOCK_DECLARE(Segment);

//mm是每种类型的table，已经分配的数量
int mm[HASH_TYPE] = {0};

void *add_pmalloc(int type, size_t size, size_t *mapped_len)
{
	char path[100];
	switch (type)
	{
	case HASH_DIRECTORY:
		mm[HASH_DIRECTORY]++;
		sprintf(path, "/mnt/dax/DIRECTORY_%d", mm[HASH_DIRECTORY]);
		break;
	case HASH_SEGMENT:
		mm[HASH_SEGMENT]++;
		sprintf(path, "/mnt/dax/SEGMENT_%d", mm[HASH_SEGMENT]);
		break;
	// case ARRAY:
	// 	mm[ARRAY] = __sync_fetch_and_add(&mm[ARRAY], 1);
	// 	sprintf(path, "/mnt/dax/ARRAY_%d", mm[ARRAY]);
	// 	break;
	default:
		printf("add_pmalloc: wrong type!\n");
		return NULL;
	}

	void *addr;
	int is_pmem;

	if ((addr = pmem_map_file(path, size, PMEM_FILE_CREATE,
							  0666, mapped_len, &is_pmem)) == NULL)
	{
		return NULL;
	}

	return addr;
}

void *getNode(int type)
{
	switch (type)
	{
	case HASH_DIRECTORY:;
		NVMBLOCK(Directory) *nvm_Block_Directory = table[type][mm[type]];
		if (nvm_Block_Directory == NULL || nvm_Block_Directory->used_num == MEMORY_BLOCK_LEN(Directory))
		{
			nvm_Block_Directory = getNvmBlock(type);
			nvm_Block_Directory->used_num = 0;
		}
		//nvm_Block->used_num++;
		return &nvm_Block_Directory->data[(nvm_Block_Directory->used_num++)%2];
	case HASH_SEGMENT:;
		NVMBLOCK(Segment) *nvm_Block_Segment = table[type][mm[type]];
		
		if (nvm_Block_Segment == NULL || nvm_Block_Segment->used_num == MEMORY_BLOCK_LEN(Segment))
		{
			nvm_Block_Segment = getNvmBlock(type);
			nvm_Block_Segment->used_num = 0;
		}
		//printf("%u %u\n",nvm_Block_Segment->used_num,MEMORY_BLOCK_LEN(Segment));
		//nvm_Block->used_num++;
		return &nvm_Block_Segment->data[nvm_Block_Segment->used_num++];
	default:
		printf("add_pmalloc: wrong type!\n");
		return NULL;
	}
}

void *getNvmBlock(int type)
{
	size_t mapped_len;
	void *newBlock = add_pmalloc(type, BLOCK_SIZE, &mapped_len);
	if (!newBlock)
	{
		printf("newBlock creation fails: nvm\n");
		exit(1);
	}

	table[type][mm[type]] = newBlock;
	return newBlock;
}

void mfence()
{
    asm volatile("mfence" ::
                     : "memory");
}
