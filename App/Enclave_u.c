#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_randomness_generation_t {
	int ms_type;
	int ms_n;
	int ms_M;
	int ms_d;
	int ms_L;
	uint8_t* ms_buffer;
	size_t ms_len;
} ms_randomness_generation_t;

static const struct {
	size_t nr_ocall;
	void * table[1];
} ocall_table_Enclave = {
	0,
	{ NULL },
};
sgx_status_t randomness_generation(sgx_enclave_id_t eid, int type, int n, int M, int d, int L, uint8_t* buffer, size_t len)
{
	sgx_status_t status;
	ms_randomness_generation_t ms;
	ms.ms_type = type;
	ms.ms_n = n;
	ms.ms_M = M;
	ms.ms_d = d;
	ms.ms_L = L;
	ms.ms_buffer = buffer;
	ms.ms_len = len;
	status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
	return status;
}

