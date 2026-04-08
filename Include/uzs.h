#ifndef UZS_H
#define UZS_H
#include "sgx_urts.h"
#include "public_function.h"
void run_uzs(sgx_enclave_id_t eid, int n, int m, int d, const std::vector<std::vector<int>>& p_sets);
#endif