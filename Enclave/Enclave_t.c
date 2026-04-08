#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define ADD_ASSIGN_OVERFLOW(a, b) (	\
	((a) += (b)) < (b)	\
)


typedef struct ms_randomness_generation_t {
	int ms_type;
	int ms_n;
	int ms_M;
	int ms_d;
	int ms_L;
	uint8_t* ms_buffer;
	size_t ms_len;
} ms_randomness_generation_t;

static sgx_status_t SGX_CDECL sgx_randomness_generation(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_randomness_generation_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_randomness_generation_t* ms = SGX_CAST(ms_randomness_generation_t*, pms);
	ms_randomness_generation_t __in_ms;
	if (memcpy_s(&__in_ms, sizeof(ms_randomness_generation_t), ms, sizeof(ms_randomness_generation_t))) {
		return SGX_ERROR_UNEXPECTED;
	}
	sgx_status_t status = SGX_SUCCESS;
	uint8_t* _tmp_buffer = __in_ms.ms_buffer;
	size_t _tmp_len = __in_ms.ms_len;
	size_t _len_buffer = _tmp_len * sizeof(uint8_t);
	uint8_t* _in_buffer = NULL;

	if (sizeof(*_tmp_buffer) != 0 &&
		(size_t)_tmp_len > (SIZE_MAX / sizeof(*_tmp_buffer))) {
		return SGX_ERROR_INVALID_PARAMETER;
	}

	CHECK_UNIQUE_POINTER(_tmp_buffer, _len_buffer);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_buffer != NULL && _len_buffer != 0) {
		if ( _len_buffer % sizeof(*_tmp_buffer) != 0)
		{
			status = SGX_ERROR_INVALID_PARAMETER;
			goto err;
		}
		if ((_in_buffer = (uint8_t*)malloc(_len_buffer)) == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		memset((void*)_in_buffer, 0, _len_buffer);
	}
	randomness_generation(__in_ms.ms_type, __in_ms.ms_n, __in_ms.ms_M, __in_ms.ms_d, __in_ms.ms_L, _in_buffer, _tmp_len);
	if (_in_buffer) {
		if (memcpy_verw_s(_tmp_buffer, _len_buffer, _in_buffer, _len_buffer)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}

err:
	if (_in_buffer) free(_in_buffer);
	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv; uint8_t is_switchless;} ecall_table[1];
} g_ecall_table = {
	1,
	{
		{(void*)(uintptr_t)sgx_randomness_generation, 0, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
} g_dyn_entry_table = {
	0,
};


