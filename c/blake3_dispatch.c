#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "blake3.h"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || defined(_M_X64)
#define IS_X86 
#endif

#if defined(__arm__)
#define IS_ARM
#endif

#if defined(IS_X86)
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__)
#include <immintrin.h>
#else
#error "Unimplemented!"
#endif
#endif


// Declarations for implementation-specific functions.
void blake3_compress_in_place_portable(uint32_t cv[8],
                                       const uint8_t block[BLAKE3_BLOCK_LEN],
                                       uint8_t block_len, uint64_t counter,
                                       uint8_t flags);

void blake3_compress_xof_portable(const uint32_t cv[8],
                                  const uint8_t block[BLAKE3_BLOCK_LEN],
                                  uint8_t block_len, uint64_t counter,
                                  uint8_t flags, uint8_t out[64]);

void blake3_hash_many_portable(const uint8_t *const *inputs, size_t num_inputs,
                               size_t blocks, const uint32_t key[8],
                               uint64_t counter, bool increment_counter,
                               uint8_t flags, uint8_t flags_start,
                               uint8_t flags_end, uint8_t *out);


#if defined(IS_X86)
#if !defined(BLAKE3_NO_SSE41)
void blake3_compress_in_place_sse41(uint32_t cv[8],
                                    const uint8_t block[BLAKE3_BLOCK_LEN],
                                    uint8_t block_len, uint64_t counter,
                                    uint8_t flags);
void blake3_compress_xof_sse41(const uint32_t cv[8],
                               const uint8_t block[BLAKE3_BLOCK_LEN],
                               uint8_t block_len, uint64_t counter,
                               uint8_t flags, uint8_t out[64]);
void blake3_hash_many_sse41(const uint8_t *const *inputs, size_t num_inputs,
                            size_t blocks, const uint32_t key[8],
                            uint64_t counter, bool increment_counter,
                            uint8_t flags, uint8_t flags_start,
                            uint8_t flags_end, uint8_t *out);
#endif                               
#if !defined(BLAKE3_NO_AVX2)
void blake3_hash_many_avx2(const uint8_t *const *inputs, size_t num_inputs,
                           size_t blocks, const uint32_t key[8],
                           uint64_t counter, bool increment_counter,
                           uint8_t flags, uint8_t flags_start,
                           uint8_t flags_end, uint8_t *out);
#endif
#if !defined(BLAKE3_NO_AVX512)
void blake3_compress_in_place_avx512(uint32_t cv[8],
                                     const uint8_t block[BLAKE3_BLOCK_LEN],
                                     uint8_t block_len, uint64_t counter,
                                     uint8_t flags);


void blake3_compress_xof_avx512(const uint32_t cv[8],
                                const uint8_t block[BLAKE3_BLOCK_LEN],
                                uint8_t block_len, uint64_t counter,
                                uint8_t flags, uint8_t out[64]);

void blake3_hash_many_avx512(const uint8_t *const *inputs, size_t num_inputs,
                             size_t blocks, const uint32_t key[8],
                             uint64_t counter, bool increment_counter,
                             uint8_t flags, uint8_t flags_start,
                             uint8_t flags_end, uint8_t *out);
#endif
#endif

#if defined(IS_ARM) && !defined(BLAKE3_NO_NEON)
void blake3_hash_many_neon(const uint8_t *const *inputs, size_t num_inputs,
                           size_t blocks, const uint32_t key[8],
                           uint64_t counter, bool increment_counter,
                           uint8_t flags, uint8_t flags_start,
                           uint8_t flags_end, uint8_t *out);
#endif

#if defined(IS_X86)
static uint64_t xgetbv()
{
#if defined(_MSC_VER)
    return _xgetbv(0);
#else
    uint32_t eax=0, edx=0;
    __asm__ __volatile__("xgetbv\n" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((uint64_t)edx << 32) | eax;
#endif
}

static void cpuid(uint32_t out[4], uint32_t id) 
{
#if defined(_MSC_VER)
    __cpuid((int*)out, id);
#else
#if defined(__i386__) || defined(_M_IX86)
    __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(out[0]), "=S"(out[1]), "=c"(out[2]), "=d"(out[3]) : "a"(id));
#else
    __asm__ __volatile__("cpuid\n" : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3]) : "a"(id));
#endif
#endif
}

static void cpuidex(uint32_t out[4], uint32_t id, uint32_t sid) 
{
#if defined(_MSC_VER)
    __cpuidex((int*)out, id, sid);
#else
#if defined(__i386__) || defined(_M_IX86)
    __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(out[0]), "=S"(out[1]), "=c"(out[2]), "=d"(out[3]) : "a"(id), "c"(sid));
#else
    __asm__ __volatile__("cpuid\n" : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3]) : "a"(id), "c"(sid));
#endif
#endif
}

#endif

enum cpu_feature {
    SSE2     = 1 << 0,
    SSSE3    = 1 << 1,
    SSE41    = 1 << 2,
    AVX      = 1 << 3,
    AVX2     = 1 << 4,
    AVX512F  = 1 << 5,
    AVX512VL = 1 << 6,
    /* ... */
    UNDEFINED = 1 << 30
};

#if !defined(BLAKE3_TESTING)
static /* Allow the variable to be controlled manually for testing */
#endif
enum cpu_feature g_cpu_features = UNDEFINED;

#if !defined(BLAKE3_TESTING)
static 
#endif
enum cpu_feature get_cpu_features()
{
    
    if( g_cpu_features != UNDEFINED ) {
        return g_cpu_features;
    } else {
#if defined(IS_X86)
        uint32_t regs[4] = {0};
        uint32_t * eax = &regs[0], * ebx = &regs[1], * ecx = &regs[2], * edx = &regs[3];
        (void)edx;
        enum cpu_feature features = 0;
        cpuid(regs, 0);
        const int max_id = *eax;
        cpuid(regs, 1);
    #if defined(__amd64__) || defined(_M_X64)
        features |= SSE2;
    #else
        if(*edx & (1UL << 26))
            features |= SSE2;
    #endif
        if(*ecx & (1UL << 0))
            features |= SSSE3;
        if(*ecx & (1UL << 19))
            features |= SSE41;

        if( *ecx & (1UL << 27) ) { // OSXSAVE
            const uint64_t mask = xgetbv();
            if( (mask & 6) == 6 ) { // SSE and AVX states
                if(*ecx & (1UL << 28))
                    features |= AVX;
                if(max_id >= 7) {
                    cpuidex(regs, 7, 0);
                    if( *ebx & (1UL << 5) )
                        features |= AVX2;
                    if( (mask & 224) == 224 ) { // Opmask, ZMM_Hi256, Hi16_Zmm
                        if( *ebx & (1UL << 31) )
                            features |= AVX512VL;
                        if(*ebx & (1UL << 16))
                            features |= AVX512F;
                    }
                }
            }
        }
        g_cpu_features = features;
        return features;
#elif defined(IS_ARM)
        /* How to detect NEON? */
        return 0;
#else
        return 0;
#endif
    }
}

void blake3_compress_in_place(uint32_t cv[8], 
                              const uint8_t block[BLAKE3_BLOCK_LEN], 
                              uint8_t block_len, uint64_t counter, 
                              uint8_t flags)
{
    const enum cpu_feature features = get_cpu_features();
#if defined(IS_X86)
#if !defined(BLAKE3_NO_AVX512)
    if(features & AVX512VL) {
        blake3_compress_in_place_avx512(cv, block, block_len, counter, flags);
        return;
    } 
#endif
#if !defined(BLAKE3_NO_SSE41)
    if(features & SSE41) {
        blake3_compress_in_place_sse41(cv, block, block_len, counter, flags);
        return;
    }
#endif
#endif
    blake3_compress_in_place_portable(cv, block, block_len, counter, flags);
}

void blake3_compress_xof(const uint32_t cv[8],
                        const uint8_t block[BLAKE3_BLOCK_LEN],
                        uint8_t block_len, uint64_t counter,
                        uint8_t flags, uint8_t out[64])
{
    const enum cpu_feature features = get_cpu_features();
#if defined(IS_X86)
#if !defined(BLAKE3_NO_AVX512)
    if(features & AVX512VL) {
        blake3_compress_xof_avx512(cv, block, block_len, counter, flags, out);
        return;
    }
#endif
#if !defined(BLAKE3_NO_SSE41)
    if(features & SSE41) {
        blake3_compress_xof_sse41(cv, block, block_len, counter, flags, out);
        return;
    }
#endif
#endif
    blake3_compress_xof_portable(cv, block, block_len, counter, flags, out);
}

void blake3_hash_many(const uint8_t *const *inputs, size_t num_inputs,
                           size_t blocks, const uint32_t key[8],
                           uint64_t counter, bool increment_counter,
                           uint8_t flags, uint8_t flags_start,
                           uint8_t flags_end, uint8_t *out)
{
    const enum cpu_feature features = get_cpu_features();
#if defined(IS_X86)
#if !defined(BLAKE3_NO_AVX512)
    if(features & AVX512F) {
        blake3_hash_many_avx512(inputs, num_inputs, blocks, key, counter, increment_counter, flags, flags_start, flags_end, out);
        return;
    }
#endif
#if !defined(BLAKE3_NO_AVX2)
    if(features & AVX2) {
        blake3_hash_many_avx2(inputs, num_inputs, blocks, key, counter, increment_counter, flags, flags_start, flags_end, out);
        return;
    }
#endif
#if !defined(BLAKE3_NO_SSE41)
    if(features & SSE41) {
        blake3_hash_many_sse41(inputs, num_inputs, blocks, key, counter, increment_counter, flags, flags_start, flags_end, out);
        return;
    }
#endif
#endif
    blake3_hash_many_portable(inputs, num_inputs, blocks, key, counter, increment_counter, flags, flags_start, flags_end, out);
}

