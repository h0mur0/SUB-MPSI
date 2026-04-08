#include "Enclave.h"
#include "Enclave_t.h" 
#include "sgx_trts.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vector>

// 辅助：生成随机 uint8 缓冲
void fill_rand(uint8_t* ptr, size_t len, int L) {
    sgx_read_rand(ptr, len);
    for(size_t i = 0; i < len; ++i) {
        ptr[i] = ptr[i] % L;
    }
}

/**
 * type: 0-SPIR, 1-UZS, 2-IT-MPSI
 * 参数 buffer 用于传回打包好的随机数
 */
void randomness_generation(int type, int n, int M, int d, int L, uint8_t* buffer, size_t len) {
    if (type == 0) { // SPIR
        // 需要 r_s(M) 和 r_c(M)
        fill_rand(buffer, M, L);         // r_s
        fill_rand(buffer + M, M, L);     // r_c
    } 
    else if (type == 1) { // UZS
        // 需要 R(n, d) 且 sum(R_i) = 0, 以及 c(1)
        uint8_t* R_ptr = buffer;
        std::vector<uint8_t> sum_R(d, 0);
        
        for (int i = 0; i < n - 1; ++i) {
            fill_rand(R_ptr + i * d, d, L);
            for (int j = 0; j < d; ++j) 
                sum_R[j] = (sum_R[j] + R_ptr[i * d + j]) % L;
        }
        // 计算最后一个 R[n-1] 使得和为 0
        for (int j = 0; j < d; ++j) 
            R_ptr[(n - 1) * d + j] = (L - sum_R[j]) % L;

        // 生成 c (1~L-1)
        uint8_t c;
        do { sgx_read_rand(&c, 1); c %= L; } while (c == 0);
        buffer[n * d] = c;
    }
    else if (type == 2) { // IT-MPSI
        // 需要 R_c(d), R_s(n-1, d), T(n-1, d) [sum T=0], c(1)
        uint8_t* ptr = buffer;
        fill_rand(ptr, d, L); ptr += d;           // R_c
        
        // R_s
        fill_rand(ptr, (n - 1) * d, L); ptr += (n - 1) * d; 
        
        // T with sum constraint
        std::vector<uint8_t> sum_T(d, 0);
        for (int i = 0; i < n - 2; ++i) {
            fill_rand(ptr + i * d, d, L);
            for (int j = 0; j < d; ++j) 
                sum_T[j] = (sum_T[j] + ptr[i * d + j]) % L;
        }
        for (int j = 0; j < d; ++j) 
            ptr[(n - 2) * d + j] = (L - sum_T[j]) % L;
        ptr += (n - 1) * d;

        // c
        uint8_t c;
        do { sgx_read_rand(&c, 1); c %= L; } while (c == 0);
        *ptr = c;
    }
}