#ifndef SPIR_H
#define SPIR_H
#include "sgx_urts.h"
#include "public_function.h"
void run_spir(sgx_enclave_id_t eid, int M, int q, int b, const std::vector<int>& db, const std::vector<int>& ind);
#endif