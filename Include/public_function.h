#ifndef PUBLIC_FUNCTION_H
#define PUBLIC_FUNCTION_H

#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <random>

// 统计结构体
struct Stats {
    std::string name;
    double duration_ms = 0.0;
    uint64_t sent_bytes = 0;
    uint64_t recv_bytes = 0;
    void print() const;
};

// 基础数学与数据集生成
bool is_prime(int n);
int find_next_prime(int n);
std::vector<int> generate_random_vector(int L, int K);
std::vector<int> get_random_subset(int M, int b);
std::vector<int> vec_add(const std::vector<int>& a, const std::vector<int>& b, int q);

// IT-MPSI 特有数据集生成
std::vector<std::vector<int>> generate_datasets_with_intersection(int n, int m, int d, int i_size);

#endif