#ifndef ENCLAVE_U_H__
#define ENCLAVE_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_status_t etc. */

#include "user_types.h"

#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif


sgx_status_t randomness_generation(sgx_enclave_id_t eid, int type, int n, int M, int d, int L, uint8_t* buffer, size_t len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
