#include "new_hash.h"
#include <time.h>
int main(int argc, char *argv[])
{
    long long TEST_NUM = atoi(argv[1]);
    long long radio = atoi(argv[2]);
    printf("This test insert number is :%d\n", TEST_NUM);
    printf("This test radio is:%d\n", radio);

    long long RANGE_NUM = TEST_NUM * 10 / radio;
    long long POSITIVE_SEARCH_NUM = TEST_NUM * radio / 10;

    u64 *range = malloc(sizeof(u64) * RANGE_NUM);
    u64 *search_array = malloc(sizeof(u64) * TEST_NUM);
    u64 i;
    for (i = 0; i < RANGE_NUM; ++i)
        range[i] = i;

    printf("insert begin \n");
    srand(NULL);
    for (i = 0; i < RANGE_NUM; ++i)
    {
        u64 temp = range[i];
        u64 j = rand() % RANGE_NUM;
        range[i] = range[j];
        range[j] = temp;
    }
    for (i = 0; i < POSITIVE_SEARCH_NUM; ++i)
    {
        u64 j = rand() % TEST_NUM;
        search_array[i] = range[j];
    }
    for (i = POSITIVE_SEARCH_NUM; i < TEST_NUM; i++)
    {
        u64 j = rand() % (RANGE_NUM - TEST_NUM) + TEST_NUM;
        search_array[i] = range[j];
    }

    HASH o_hash; //origin hash
    init_hash(&o_hash);
    struct timeval start, end;
    long long time_consumption = 0;
    mfence();
    gettimeofday(&start, NULL);
    mfence();
    for (i = 0; i < TEST_NUM; ++i)
    {
        #ifdef DEBUG_ERROR
        printf("%llx   insert %x \n",i , range[i]);
        #endif
        
        int ok = insert_hash(&o_hash, range[i], i + 1);
        
        #ifdef DEBUG_ERROR
        int j;
        for (j = 0; j < i; ++j)
        {
            if (search_hash(&o_hash, range[j]) != (j + 1))
            {
                printf("search %x error search_result=%x\n", range[j], search_hash(&o_hash, range[j]));
                return 0;
            }
        }
        #endif
    }
    mfence();
    gettimeofday(&end, NULL);
    mfence();
    time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("time_consumption of insert is %lld\n", time_consumption);
    #ifdef DEBUG_TIME
        print_resize();
    #endif

    printf("insert all over\n");
    int j;
    mfence();
    gettimeofday(&start, NULL);
    mfence();
    for (j = 0; j < TEST_NUM; ++j)
    {
        search_hash(&o_hash, search_array[j]);
    }
    mfence();
    gettimeofday(&end, NULL);
    mfence();
    time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("time_consumption of search is %lld\n", time_consumption);
    return 0;
}
