#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

int main ()
{

    // how many small allocations should be performed instead of 1 big one of the same total size.
    std::vector<int> n_alloc_sizes{1,10,100,1000,10000,100000,1000000};

    for(int n_small_allocs : n_alloc_sizes){
        int i;
        void* bigPtr;
        void* ptrArray[n_small_allocs];

        clock_t t1, t2, t3;

        t1 = clock();
        for (i = 0; i < n_small_allocs; ++i)
        {
            ptrArray[i] = malloc(2048);
        }
        t2 = clock();

        bigPtr = malloc(n_small_allocs*2048);

        t3 = clock();

        printf("1 big allocation: %2.0f ms,    %12i small allocations = %8.0f ms\n",
            difftime(t3, t2), n_small_allocs, difftime(t2, t1));
    }

    return 0;
}
