#include "public_function.h"

void Stats::print() const {
        double sent_mb = sent_bytes / (1024.0 * 1024.0);
        double recv_mb = recv_bytes / (1024.0 * 1024.0);

        std::cout << std::left << std::setw(15) << name 
                  << " | Time: " << std::fixed << std::setprecision(3) << std::setw(8) << duration_ms << " ms"
                  << " | Sent: " << std::setw(10) << sent_mb << " MB"
                  << " | Recv: " << std::setw(10) << recv_mb << " MB" << std::endl;
}

bool is_prime(int n) {
    if (n <= 1) return false;
    for (int i = 2; i * i <= n; i++) if (n % i == 0) return false;
    return true;
}

int find_next_prime(int n) {
    int l = n + 1;
    while (!is_prime(l)) l++;
    return l;
}

std::vector<int> generate_random_vector(int L, int K) {
    std::vector<int> res(K);
    for (int i = 0; i < K; i++) res[i] = rand() % L;
    return res;
}

std::vector<int> get_random_subset(int M, int b) {
    std::vector<int> indices(M);
    std::iota(indices.begin(), indices.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);
    if (b < M) indices.resize(b);
    return indices;
}

std::vector<int> vec_add(const std::vector<int>& a, const std::vector<int>& b, int q) {
    std::vector<int> res(a.size());
    for (size_t i = 0; i < a.size(); ++i) res[i] = (a[i] + b[i]) % q;
    return res;
}

std::vector<std::vector<int>> generate_datasets_with_intersection(int n, int m, int d, int i_size) {
    if ((n * (m-i_size) + i_size > d) || (m < i_size)){
        std::cout << "Invalid parameters!" << std::endl;
        exit(0);
    }
    std::vector<int> universe(d);
    std::iota(universe.begin(), universe.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(universe.begin(), universe.end(), g);

    std::vector<int> inter(universe.begin(), universe.begin() + i_size);
    std::vector<int> pool(universe.begin() + i_size, universe.end());
    
    std::vector<std::vector<int>> sets(n);
    int extra = m - i_size;
    for (int p = 0; p < n; ++p) {
        sets[p] = inter;
        for (int k = 0; k < extra; ++k) sets[p].push_back(pool[p * extra + k]);
    }
    return sets;
}