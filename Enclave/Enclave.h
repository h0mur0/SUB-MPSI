#ifndef _ENCLAVE_H_
#define _ENCLAVE_H_

#include <assert.h>
#include <stdlib.h>
#include "sgx_trts.h"

#if defined(__cplusplus)
extern "C" {
#endif
void randomness_generation(int type, int n, int M, int d, int L, uint8_t* buffer, size_t len);

#if defined(__cplusplus)
}
#endif

#endif /* !_ENCLAVE_H_ */
