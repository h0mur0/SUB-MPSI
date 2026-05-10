#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstdint>
#include "coproto/coproto.h"
#include "Enclave_u.h" 
#include "public_function.h"

using namespace coproto;
using namespace std::chrono;

task<void> enclave_protocol_proxy(sgx_enclave_id_t eid, Socket& to_leader, std::vector<std::array<Socket*, 2>>& to_servers, int n, int L, int d, Stats& stats) {
    auto start = high_resolution_clock::now();
    
    // 1. 计算所需缓冲区长度: Rc(d) + (n-1)*Rs(d) + (n-1)*T(d) + c(1)
    size_t buffer_len = (size_t)d + (size_t)(n - 1) * d + (size_t)(n - 1) * d + 1;
    std::vector<uint8_t> buffer(buffer_len,0);
    

    // 2. 调用 Enclave 获取安全随机数 (协议类型 2 代表 IT-MPSI)
    sgx_status_t res = randomness_generation(eid, 2, n, 0, d, L, buffer.data(), (uint32_t)buffer_len);
    if (res != SGX_SUCCESS) {
        std::cerr << "SGX ECALL Failed!" << std::endl;
        co_return;
    }

    auto t1 = high_resolution_clock::now();
    auto time1 = duration_cast<microseconds>(t1 - start).count() / 1000.0;
    std::cout << "get randomness time: " << time1 << std::endl;

    // 3. 解析缓冲区
    auto it = buffer.begin();
    std::vector<uint8_t> R_c(it, it + d); it += d;
    
    std::vector<std::vector<uint8_t>> R_s(n - 1), T(n - 1);
    for(int i=0; i < n-1; ++i) { R_s[i].assign(it, it + d); it += d; }
    for(int i=0; i < n-1; ++i) { T[i].assign(it, it + d); it += d; }
    uint8_t c = *it;

    auto t2 = high_resolution_clock::now();
    auto time2 = duration_cast<microseconds>(t2 - t1).count() / 1000.0;
    std::cout << "analyse time: " << time2 << std::endl;

    // 4. 分发逻辑 (保持你原有的 Socket 逻辑)
    co_await to_leader.send(R_c);
    for (int i = 0; i < n - 1; ++i) {
        // 分发给双 Server 结构
        co_await to_servers[i][0]->send(R_s[i]);
        co_await to_servers[i][0]->send(T[i]);
        co_await to_servers[i][0]->send(c);
        co_await to_servers[i][0]->send(R_c);
        
        co_await to_servers[i][1]->send(R_s[i]);
        co_await to_servers[i][1]->send(T[i]);
        co_await to_servers[i][1]->send(c);
    }

    auto t3 = high_resolution_clock::now();
    auto time3 = duration_cast<microseconds>(t3 - t2).count() / 1000.0;
    std::cout << "distribution time: " << time3 << std::endl;

    auto end = high_resolution_clock::now();
    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    stats.sent_bytes = to_leader.bytesSent();
    for(auto& pair : to_servers) stats.sent_bytes += (pair[0]->bytesSent() + pair[1]->bytesSent());
}

/** Server 协议 (逻辑完全保持你提供的代码) **/
task<void> server_protocol(Socket& from_e, Socket& from_l, const std::vector<int>& privacy_set, int type, int L, int d, Stats& stats) {
    auto start = high_resolution_clock::now();
    std::vector<uint8_t> R_si(d), T_i(d), R_c(d), Q_i2(d);
    uint8_t c;

    co_await from_e.recv(R_si);
    co_await from_e.recv(T_i);
    co_await from_e.recv(c);

    if (type == 1) co_await from_e.recv(R_c);
    else co_await from_l.recv(Q_i2);

    std::vector<uint8_t> W_i(d, 1);
    for (int x : privacy_set) if(x < d) W_i[x] = 0;

    std::vector<uint8_t> A_ij(d);
    for (int j = 0; j < d; ++j) {
        int common = (static_cast<int>(c) * (W_i[j] + T_i[j])) % L;
        int base = (type == 1) ? R_c[j] : (int)Q_i2[j];
        A_ij[j] = static_cast<uint8_t>((common * base + R_si[j]) % L);
    }

    co_await from_l.send(A_ij);
    auto end = high_resolution_clock::now();
    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    stats.sent_bytes = from_e.bytesSent() + from_l.bytesSent();
    stats.recv_bytes = from_e.bytesReceived() + from_l.bytesReceived();
}

/** Leader 协议 (逻辑完全保持你提供的代码) **/
task<void> leader_protocol(Socket& from_e, std::vector<std::array<Socket*, 2>>& to_s, const std::vector<int>& p_set, int n, int L, int d, int expected_i, Stats& stats) {
    auto start = high_resolution_clock::now();
    std::vector<uint8_t> R_c(d);

    co_await from_e.recv(R_c);
    std::vector<uint8_t> V(d, 0);
    for (int x : p_set) if(x < d) V[x] = 1;
    
    // 向量模加
    std::vector<uint8_t> Q_i2(d);
    for (int j = 0; j < d; ++j) Q_i2[j] = (R_c[j] + V[j]) % L;

    for (int i = 0; i < n - 1; ++i) co_await to_s[i][1]->send(Q_i2);

    std::vector<std::vector<uint8_t>> D(n - 1, std::vector<uint8_t>(d));
    for (int i = 0; i < n - 1; ++i) {
        std::vector<uint8_t> A_i1(d), A_i2(d);
        co_await to_s[i][0]->recv(A_i1);
        co_await to_s[i][1]->recv(A_i2);
        for (int j = 0; j < d; ++j) {
            D[i][j] = static_cast<uint8_t>((static_cast<int>(A_i2[j]) - A_i1[j] + L) % L);
        }
    }

    std::vector<int> result;
    for (int x : p_set) {
        if(x >= d) continue;
        int sum_dx = 0;
        for (int i = 0; i < n - 1; ++i) sum_dx += D[i][x];
        if (sum_dx % L == 0) result.push_back(x);
    }

    auto end = high_resolution_clock::now();
    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    
    std::cout << std::string(85, '=') << std::endl;
    std::cout << "[IT-MPSI Result]" << std::endl;
    std::cout << "Expected Intersection Size: " << expected_i << std::endl;
    std::cout << "Actual Intersection Size:   " << result.size() << std::endl;
    std::cout << "Status: " << (result.size() == (size_t)expected_i ? "SUCCESS" : "FAILED") << std::endl;

    stats.sent_bytes = from_e.bytesSent();
    for(auto& p : to_s) stats.sent_bytes += (p[0]->bytesSent() + p[1]->bytesSent());
    stats.recv_bytes = from_e.bytesReceived();
    for(auto& p : to_s) stats.recv_bytes += (p[0]->bytesReceived() + p[1]->bytesReceived());
}

void run_itmpsi(sgx_enclave_id_t eid, int n, int m, int d, int i_val, const std::vector<std::vector<int>>& privacy_sets) {
    int L = find_next_prime(n-1);

    Stats eStats{"Enclave_SGX"}, lStats{"Leader"};
    std::vector<std::array<Stats, 2>> sStats(n - 1);
    
    auto e_l = LocalAsyncSocket::makePair();
    std::vector<std::array<std::array<LocalAsyncSocket, 2>, 2>> e_s_chans(n-1), l_s_chans(n-1);
    std::vector<std::array<Socket*, 2>> e_to_s(n-1), l_to_s(n-1);
    std::vector<task<void>> tasks;

    for (int i = 0; i < n - 1; ++i) {
        sStats[i][0].name = "Server " + std::to_string(i+1) + ",1";
        sStats[i][1].name = "Server " + std::to_string(i+1) + ",2";
        e_s_chans[i][0] = LocalAsyncSocket::makePair(); e_s_chans[i][1] = LocalAsyncSocket::makePair();
        l_s_chans[i][0] = LocalAsyncSocket::makePair(); l_s_chans[i][1] = LocalAsyncSocket::makePair();
        e_to_s[i] = {&e_s_chans[i][0][0], &e_s_chans[i][1][0]};
        l_to_s[i] = {&l_s_chans[i][0][0], &l_s_chans[i][1][0]};
    }

    auto start_all = high_resolution_clock::now();
    
    // 任务注入
    tasks.push_back(enclave_protocol_proxy(eid, e_l[0], e_to_s, n, L, d, eStats));
    tasks.push_back(leader_protocol(e_l[1], l_to_s, privacy_sets[n-1], n, L, d, i_val, lStats));
    
    for (int i = 0; i < n - 1; ++i) {
        tasks.push_back(server_protocol(e_s_chans[i][0][1], l_s_chans[i][0][1], privacy_sets[i], 1, L, d, sStats[i][0]));
        tasks.push_back(server_protocol(e_s_chans[i][1][1], l_s_chans[i][1][1], privacy_sets[i], 2, L, d, sStats[i][1]));
    }
    
    sync_wait(when_all_ready(std::move(tasks)));
    auto end_all = high_resolution_clock::now();

    std::cout << std::string(85, '-') << std::endl;
    eStats.print(); 
    lStats.print();

    uint64_t total_sent_bytes = eStats.sent_bytes + lStats.sent_bytes;

    for(int i=0; i<n-1; ++i) { 
        sStats[i][0].print(); 
        sStats[i][1].print(); 
        total_sent_bytes += (sStats[i][0].sent_bytes + sStats[i][1].sent_bytes);
    }

    double total_comm_mb = total_sent_bytes / (1024.0 * 1024.0);

    std::cout << std::string(85, '-') << std::endl;
    std::cout << "Total Runtime      : " << std::fixed << std::setprecision(3) 
              << duration_cast<microseconds>(end_all - start_all).count() / 1000.0 << " ms" << std::endl;
    std::cout << "Total Communication: " << std::fixed << std::setprecision(4) 
              << total_comm_mb << " MB" << std::endl;
    std::cout << std::string(85, '=') << std::endl;
}