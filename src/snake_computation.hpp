#include <vector>
#include <immintrin.h> // Vector intrinsics

inline static void compute_end_of_snake(int &x, int &y, const std::vector<int> &in_1, const std::vector<int> &in_2)
{

    int in1_size = in_1.size();
    int in2_size = in_2.size();

#ifndef SIMD
    while (x < in1_size && y < in2_size && in_1.at(x) == in_2.at(y))
    {
        x++;
        y++;
    }

#else
    // SIMD no extract
    if (x < in1_size && y < in2_size && in_1.at(x) == in_2.at(y))
    {
        // already checked in if
        x++;
        y++;

        // hopefully computed before
        __m256i ones = _mm256_set1_epi64x(-1); // compiler optimized
        __m128i ones_sse = _mm_set1_epi64x(-1);

        // assuming 32-bit integers
        while (x + 7 < in1_size && y + 7 < in2_size)
        {
            // AVX = 8-way int32; cmpeq needs AVX2!
            __m256i in_1_v1 = _mm256_loadu_si256((const __m256i_u *)(in_1.data() + x)); // uops.info for VMOVDQU (YMM, M256): Haswell: throughput = 2, latency = 6-7
            __m256i in_2_v1 = _mm256_loadu_si256((const __m256i_u *)(in_2.data() + y)); //                                    AMD Zen2: throughput = 2, latency = 8
                                                                                        //  => can be performed in parallel

            __m256i cmp1 = _mm256_cmpeq_epi32(in_1_v1, in_2_v1); // == 1 if equal     // uops.info for VPCMPEQD (YMM, YMM, YMM): Haswell: tp = 2, lat = 1; AMD Zen2: tp = 2-3, lat = 1
                                                                 //  => can be performed in parallel, but cannot not load 4 in parallel -> no direct benefit
            if (!_mm256_testc_si256(cmp1, ones))
            { // test if cmp1 not all 1s       // uops.info for VPTEST (YMM, YMM): Haswell: tp = 1, lat <= 4; AMD Zen2: tp = 1-2, lat = 1-7
                // found a match
                goto loop;
            }

            x += 8;
            y += 8;
        }

#ifndef NO_SSE
        if (x + 3 < in1_size && y + 3 < in2_size)
        {
            // SSE = 4-way int32
            __m128i in_1_v = _mm_loadu_si128((const __m128i *)(in_1.data() + x));
            __m128i in_2_v = _mm_loadu_si128((const __m128i *)(in_2.data() + y));

            __m128i cmp = _mm_cmpeq_epi32(in_1_v, in_2_v); // == 1 if equal

            if (_mm_testc_si128(cmp, ones_sse))
            { // test if cmp is all 1s => end not found
                x += 4;
                y += 4;
            }
        }
#endif

    loop:
        // check <= 8 entries sequentially for exact location
        while (x < in1_size && y < in2_size && in_1.at(x) == in_2.at(y))
        {
            x++;
            y++;
        }
    }

#endif
}