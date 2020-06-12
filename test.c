#include "hash.h"
#include <time.h>
int main(int argc, char *argv[])
{
    int TEST_NUM = atoi(argv[1]);
    printf("This test insert number is :%d\n", TEST_NUM);
    printf("insert begin \n");
    HASH o_hash; //origin hash
    init(&o_hash);
    u32 *a = malloc(sizeof(u32) * TEST_NUM);
    u32 i;
    for (i = 0; i < TEST_NUM; ++i)
        a[i] = i;

    printf("insert begin \n");
    srand(NULL);
    for (i = 0; i < TEST_NUM; ++i)
    {
        u32 temp = a[i];
        u32 j = rand() % TEST_NUM;
        a[i] = a[j];
        a[j] = temp;
    }
    struct timeval start, end;
	long long time_consumption = 0;
    mfence();
    gettimeofday(&start,NULL);
    mfence();
    for (i = 0; i < TEST_NUM; ++i)
    {
        //printf("insert %x \n", a[i]);
        int ok = insert_hash(&o_hash, a[i], i + 1);
        //printf("%d\n", o_hash.global_depth);
        // if (ok == -1)
        // {
        //     //printf("insert %x error1\n", a[i]);
        //     return 0;
        // }
        //print(&o_hash);
        /*int j;
        for (j = 0; j < i; ++j)
        {
            if (search_hash(&o_hash, a[j]) != (j + 1))
            {
                printf("search %x error search_result=%x\n", a[j], search_hash(&o_hash, a[j]));
                return 0;
            }
            //printf("search %x ok\n", a[j]);
        }*/
    }
    mfence();
    gettimeofday(&end,NULL);
    mfence();
    time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
	printf("time_consumption of insert is %lld\n", time_consumption);

    printf("insert all over\n");
    int j;
    mfence();
    gettimeofday(&start,NULL);
    mfence();
    for (j = 0; j < TEST_NUM; ++j)
    {
        //printf("search %x \n", a[j]);
        if (search_hash(&o_hash, a[j]) != (j + 1))
        {
            printf("insert %x error search_result=%x\n", a[j], search_hash(&o_hash, a[j]));
            return 0;
        }
        //printf("search %x ok\n", a[j]);
    }
    mfence();
    gettimeofday(&end,NULL);
    mfence();
    time_consumption = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
	printf("time_consumption of search is %lld\n", time_consumption);
    return 0;
}
