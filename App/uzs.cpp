#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <random>
#include <array>
#include "coproto/coproto.h"
#include "Enclave_u.h"  // SGX 生成的头文件
#include "public_function.h"

using namespace coproto;
using namespace std::chrono;

task<void> enclave_protocol_proxy(sgx_enclave_id_t eid, std::vector<Socket*> party_sockets, int L, int d, Stats& stats) {
    auto start = high_resolution_clock::now();
    int n = party_sockets.size();
    
    // 1. 计算总长度: n 组长度为 d 的随机向量 + 1 个标量 c
    // 注意：SGX 内部会保证 sum(Ri) = 0 mod L
    size_t buffer_len = (size_t)n * d + 1;
    std::vector<uint8_t> buffer(buffer_len);

    // 2. 调用 Enclave 获取随机数 (协议类型 1 代表 UZS)
    sgx_status_t res = randomness_generation(eid, 1, n, 0, d, L, buffer.data(), (uint32_t)buffer_len);
    if (res != SGX_SUCCESS) {
        std::cerr << "SGX UZS ECALL Failed!" << std::endl;
        co_return;
    }

    // 3. 解析并并行发送
    auto it = buffer.begin();
    std::vector<task<void>> send_tasks;
    send_tasks.reserve(n);

    // 提取 c (位于 buffer 最后一位)
    uint8_t c_val = buffer.back();

    for (int i = 0; i < n; ++i) {
        // 提取当前的 R_i
        std::vector<uint8_t> R_i(it, it + d);
        it += d;

        // 异步分发给第 i 个参与方
        send_tasks.push_back([](Socket* s, std::vector<uint8_t> Ri, uint8_t cv) -> task<void> {
            co_await s->send(std::move(Ri));
            co_await s->send((int)cv); // 保持与原有协议类型一致
        }(party_sockets[i], std::move(R_i), c_val));
    }

    co_await when_all_ready(std::move(send_tasks));
    auto end = high_resolution_clock::now();
    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    for (auto* s : party_sockets) {
        co_await s->flush();
        stats.sent_bytes += s->bytesSent();
        stats.recv_bytes += s->bytesReceived();
    }
}

/** Party 协议 (逻辑保持，仅将数据类型调整为 uint8_t 适配 SGX 传输) **/
task<void> party_protocol(Socket& from_e, const std::vector<int>& privacy_set, int L, int d, Stats& stats) {
    auto start = high_resolution_clock::now();
    
    std::vector<uint8_t> R_i(d);
    int c;

    co_await from_e.recv(R_i);
    co_await from_e.recv(c);

    // 计算特征向量 W_i
    std::vector<int> W_i(d, 1);
    for (int x : privacy_set) {
        if (x >= 0 && x < d) W_i[x] = 0;
    }

    // 计算本地份额 Y_i = c * (W_i + R_i) mod L
    std::vector<int> Y_i(d);
    for (int j = 0; j < d; ++j) {
        Y_i[j] = (c * (W_i[j] + (int)R_i[j])) % L;
    }

    auto end = high_resolution_clock::now();
    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    stats.sent_bytes = from_e.bytesSent();
    stats.recv_bytes = from_e.bytesReceived();
}

void run_uzs(sgx_enclave_id_t eid, int n, int m, int d, const std::vector<std::vector<int>>& privacy_sets) {
    int L = find_next_prime(n);
    
    Stats eStats{"Enclave_SGX"};
    std::vector<Stats> pStats(n);
    
    // 初始化通信通道
    std::vector<std::array<LocalAsyncSocket, 2>> channels;
    channels.reserve(n); 

    for (int i = 0; i < n; ++i) {
        pStats[i].name = "Party " + std::to_string(i);
        channels.push_back(LocalAsyncSocket::makePair());
    }

    // 收集 Enclave 端的 Socket 指针
    std::vector<Socket*> enclave_side_ptrs;
    enclave_side_ptrs.reserve(n);
    for (int i = 0; i < n; ++i) {
        enclave_side_ptrs.push_back(&channels[i][0]);
    }

    // 启动任务
    std::vector<task<void>> tasks;
    tasks.reserve(n + 1);
    
    // 注入 Enclave 代理任务
    tasks.push_back(enclave_protocol_proxy(eid, enclave_side_ptrs, L, d, eStats));
    
    // 注入各参与方任务
    for (int i = 0; i < n; ++i) {
        tasks.push_back(party_protocol(channels[i][1], privacy_sets[i], L, d, pStats[i]));
    }

    auto start_all = high_resolution_clock::now();
    sync_wait(when_all_ready(std::move(tasks)));

    auto end_all = high_resolution_clock::now();
    double total_runtime_ms = duration_cast<microseconds>(end_all - start_all).count() / 1000.0;

    // 打印性能报告
    std::cout << std::string(85, '=') << std::endl;
    std::cout << "[UZS Result]" << std::endl;
    std::cout << std::string(85, '-') << std::endl;
    eStats.print();
    uint64_t total_comm = eStats.sent_bytes;
    for (int i = 0; i < n; ++i) {
        pStats[i].print();
        total_comm += pStats[i].sent_bytes;
    }
    double total_comm_mb = total_comm / (1024.0 * 1024.0);
    std::cout << std::string(85, '-') << std::endl;
    std::cout << "Total Runtime:       " << std::fixed << std::setprecision(3) << total_runtime_ms << " ms" << std::endl;
    std::cout << "Total Communication: " << std::fixed << std::setprecision(6) << total_comm_mb << " MB" << std::endl;
    std::cout << std::string(85, '=') << std::endl;
}