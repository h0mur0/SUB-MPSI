#ifndef IT_MPSI_H
#define IT_MPSI_H
#include "sgx_urts.h"
#include "public_function.h"
void run_itmpsi(sgx_enclave_id_t eid, int n, int m, int d, int i_val, const std::vector<std::vector<int>>& p_sets);
#endif