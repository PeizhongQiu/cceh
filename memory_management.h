#ifndef MEM
#define MEM
#include <stdio.h>
#include <stdlib.h>

#define HASH_TYPE 2
// #define ARRAY 0
#define HASH_SEGMENT 0
#define HASH_DIRECTORY 1

#define _nvmb_struct
#define _nvmb_union
#define _nvmb_enum

#define BLOCK_SIZE (32*1024*1024)

#define MEMORY_BLOCK_LEN(type) ((BLOCK_SIZE - sizeof(unsigned int)) / sizeof(type))

#define NVMBLOCK(type) \
    struct _nvmb_##type##_nvmb

#define NVMBLOCK_DECLARE(type) \
    NVMBLOCK(type)             \
    {                      \
        type data[MEMORY_BLOCK_LEN(type)];   \
		unsigned int used_num;	\
    }

#define asm_mfence()                \
({                      \
    __asm__ __volatile__ ("mfence":::"memory");    \
})

void *getNvmBlock(int type);
void *getNode(int type);
void mfence(void);
#endif
