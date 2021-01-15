#include <libpmem.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef unsigned long long u64;


void clear_cache() {
    int* dummy = malloc(1024*1024*256*sizeof(int));
    for (int i=0; i<1024*1024*256; i++) {
	    dummy[i] = i;
    }

    for (int i=100;i<1024*1024*256;i++) {
	    dummy[i] = dummy[i-rand()%100] + dummy[i+rand()%100];
    }
    free(dummy);
}

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
	char path[100] = "/mnt/dax_200g/test";
	
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
    u64 _[1024][1024*16];
};

int main()
{
    size_t mapped_len;
    struct test *a = add_pmalloc(sizeof(struct test),&mapped_len);
    u64 c;

    u64 i,j,k,l;
    for(k = 0; k < 10; k++){
        struct timespec start, end;
        
        for(i = 0; i < 1024 * 16; i++){
            a->_[i/16][i] = i%16;
            pmem_persist(&a->_[i/16][i], sizeof(a->_[i/16][i]));
            mfence();
        }

        long long time_consumption = 0;
        clear_cache();
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &start);
        mfence();
        for(l = 0; l < 100; l++){
            for(i = 0; i < 1024 * 16; i++){
                c = a->_[i/16][i];
            }
            
        }
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &end);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);
        printf("time_consumption of 1-r is %lld\n", time_consumption);

        clear_cache();
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &start);
        mfence();
        for(l = 0; l < 100; l++){
            for(i = 0; i < 1024; i++){
                for(j = 0; j < 16; j++){
                    c = a->_[i][i*16 + j];
                }
            }
        }
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &end);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);
        printf("time_consumption of 2-r is %lld\n", time_consumption);

        clear_cache();
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &start);
        mfence();
        for(l = 0; l < 100; l++){
            for(i = 0; i < 1024; i++){
                for(j = 0; j < 8; j++)
                    c = a->_[i][i*16 + 2*j];
            }
        }
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &end);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);
        printf("time_consumption of 3-r is %lld\n", time_consumption);

        clear_cache();
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &start);
        mfence();
        for(l = 0; l < 100; l++){
            for(i = 0; i < 1024; i++){
                c = a->_[i][i*16];
                int left = 1, right = 15, mid = 0;
                while(left <= right){
                    mid = (left + right) >> 1;
                    c = a->_[i][i*16 + mid];
                    if(c < 6)
                        left = mid + 1;
                    else right = mid - 1;
                }
            }
        }
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &end);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);
        printf("time_consumption of 4-r is %lld\n", time_consumption);

        clear_cache();
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &start);
        mfence();
        for(l = 0; l < 100; l++){
            for(i = 0; i < 1024; i++){
                c = a->_[i][i*16 + 0];
                c = a->_[i][i*16 + 1];
                c = a->_[i][i*16 + 2];
                c = a->_[i][i*16 + 3];
                c = a->_[i][i*16 + 4];
                c = a->_[i][i*16 + 5];
                c = a->_[i][i*16 + 6];
                c = a->_[i][i*16 + 7];
                c = a->_[i][i*16 + 8];
                c = a->_[i][i*16 + 9];
                c = a->_[i][i*16 + 10];
                c = a->_[i][i*16 + 11];
                c = a->_[i][i*16 + 12];
                c = a->_[i][i*16 + 13];
                c = a->_[i][i*16 + 14];
                c = a->_[i][i*16 + 15];
            }
        }
        mfence();
        clock_gettime(CLOCK_MONOTONIC, &end);
        mfence();
        time_consumption = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);
        printf("time_consumption of 5-r is %lld\n", time_consumption);

    }
    return 0;
}
