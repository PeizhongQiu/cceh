#include <libpmem.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef unsigned long long u64;

#define asm_mfence()                \
({                      \
    __asm__ __volatile__ ("mfence":::"memory");    \
})

void mfence()
{
    asm volatile("mfence" ::
                     : "memory");
}

void *add_pmalloc(size_t size, size_t *mapped_len)
{
	char path[100] = "/mnt/dax_200g/test_2";
	
	void *addr;
	int is_pmem;

	if ((addr = pmem_map_file(path, size, PMEM_FILE_CREATE,
							  0666, mapped_len, &is_pmem)) == NULL)
	{
		return NULL;
	}

	return addr;
}

struct test{
    u64 _[1024*16];
};

int main()
{
    size_t mapped_len;
    struct test *a = add_pmalloc(sizeof(struct test),&mapped_len);
    struct timeval start, end;
    u64 c;

    u64 i,j,k,l;
    long long time_consumption = 0;
    for(k = 0; k < 10; k++){

        mfence();
        gettimeofday(&start, NULL);
        mfence();
        for(l = 0; l < 1000; l++)
            for(i = 0; i < 1024 * 16; i++){
                a->_[i] = 1;
                pmem_persist(&a->_[i], sizeof(a->_[i]));
                mfence();
            }
        mfence();
        gettimeofday(&end, NULL);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("time_consumption of 1-w is %lld\n", time_consumption);

        mfence();
        gettimeofday(&start, NULL);
        mfence();
        for(l = 0; l < 1000; l++)
            for(i = 0; i < 1024 * 16; i++){
                c = a->_[i];
            }
        mfence();
        gettimeofday(&end, NULL);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("time_consumption of 1-r is %lld\n", time_consumption);

        mfence();
        gettimeofday(&start, NULL);
        mfence();
        for(l = 0; l < 1000; l++)    
            for(i = 0; i < 1024; i++){
                for(j = 0; j < 16; j++){
                    a->_[i*16 + j] = 1;
                    pmem_persist(&a->_[i*16 + j], sizeof(a->_[i*16 + j]));
                    mfence();
                }
            }
        mfence();
        gettimeofday(&end, NULL);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("time_consumption of 2-w is %lld\n", time_consumption);

        mfence();
        gettimeofday(&start, NULL);
        mfence();
        for(l = 0; l < 1000; l++)  
            for(i = 0; i < 1024; i++){
                for(j = 0; j < 16; j++){
                    c = a->_[i*16 + j];
                }
            }
        mfence();
        gettimeofday(&end, NULL);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("time_consumption of 2-r is %lld\n", time_consumption);

        mfence();
        gettimeofday(&start, NULL);
        mfence();
        for(l = 0; l < 1000; l++)  
            for(i = 0; i < 1024; i++){
                a->_[i*16 + 0] = 1;
                pmem_persist(&a->_[i*16 + 0], sizeof(a->_[i*16 + 0]));
                mfence();
                a->_[i*16 + 2] = 2;
                pmem_persist(&a->_[i*16 + 2], sizeof(a->_[i*16 + 2]));
                mfence();
                a->_[i*16 + 4] = 3;
                pmem_persist(&a->_[i*16 + 4], sizeof(a->_[i*16 + 4]));
                mfence();
                a->_[i*16 + 6] = 4;
                pmem_persist(&a->_[i*16 + 6], sizeof(a->_[i*16 + 6]));
                mfence();
                a->_[i*16 + 8] = 5;
                pmem_persist(&a->_[i*16 + 8], sizeof(a->_[i*16 + 8]));
                mfence();
                a->_[i*16 + 10] = 6;
                pmem_persist(&a->_[i*16 + 10], sizeof(a->_[i*16 + 10]));
                mfence();
                a->_[i*16 + 12] = 7;
                pmem_persist(&a->_[i*16 + 12], sizeof(a->_[i*16 + 12]));
                mfence();
                a->_[i*16 + 14] = 8;
                pmem_persist(&a->_[i*16 + 14], sizeof(a->_[i*16 + 14]));
                mfence();
                a->_[i*16 + 1] = 9;
                pmem_persist(&a->_[i*16 + 1], sizeof(a->_[i*16 + 1]));
                mfence();
                a->_[i*16 + 3] = 10;
                pmem_persist(&a->_[i*16 + 3], sizeof(a->_[i*16 + 3]));
                mfence();
                a->_[i*16 + 5] = 11;
                pmem_persist(&a->_[i*16 + 5], sizeof(a->_[i*16 + 5]));
                mfence();
                a->_[i*16 + 7] = 12;
                pmem_persist(&a->_[i*16 + 7], sizeof(a->_[i*16 + 7]));
                mfence();
                a->_[i*16 + 9] = 13;
                pmem_persist(&a->_[i*16 + 9], sizeof(a->_[i*16 + 9]));
                mfence();
                a->_[i*16 + 11] = 14;
                pmem_persist(&a->_[i*16 + 11], sizeof(a->_[i*16 + 11]));
                mfence();
                a->_[i*16 + 13] = 15;
                pmem_persist(&a->_[i*16 + 13], sizeof(a->_[i*16 + 13]));
                mfence();
                a->_[i*16 + 15] = 16;
                pmem_persist(&a->_[i*16 + 15], sizeof(a->_[i*16 + 15]));
                mfence();
            }
        mfence();
        gettimeofday(&end, NULL);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("time_consumption of 3-w is %lld\n", time_consumption);

        mfence();
        gettimeofday(&start, NULL);
        mfence();
        for(l = 0; l < 10000; l++)  
            for(i = 0; i < 1024; i++){
                c = a->_[i*16 + 0];
                c = a->_[i*16 + 2];
                c = a->_[i*16 + 4];
                c = a->_[i*16 + 6];
                c = a->_[i*16 + 8];
                c = a->_[i*16 + 10];
                c = a->_[i*16 + 12];
                c = a->_[i*16 + 14];
                c = a->_[i*16 + 1];
                c = a->_[i*16 + 3];
                c = a->_[i*16 + 5];
                c = a->_[i*16 + 7];
                c = a->_[i*16 + 9];
                c = a->_[i*16 + 11];
                c = a->_[i*16 + 13];
                c = a->_[i*16 + 15];
            }
        mfence();
        gettimeofday(&end, NULL);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("time_consumption of 3-r is %lld\n", time_consumption);
    }
}
