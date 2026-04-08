#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <random>
#include "coproto/coproto.h"
#include "Enclave_u.h"  
#include "public_function.h"

using namespace coproto;
using namespace std::chrono;

task<void> enclave_protocol_proxy(sgx_enclave_id_t eid, Socket& to_s1, Socket& to_s2, Socket& to_c, int q, int M, Stats& stats) {
    auto start = high_resolution_clock::now();

    size_t buffer_len = (size_t)2 * M;
    std::vector<uint8_t> buffer(buffer_len);

    sgx_status_t res = randomness_generation(eid, 0, 0, M, 0, q, buffer.data(), (uint32_t)buffer_len);
    if (res != SGX_SUCCESS) {
        std::cerr << "SGX SPIR ECALL Failed!" << std::endl;
        co_return;
    }
    std::vector<int> r_s(buffer.begin(), buffer.begin() + M);
    std::vector<int> r_c(buffer.begin() + M, buffer.end());

    co_await to_s1.send(r_s);
    co_await to_s2.send(std::move(r_s));
    co_await to_c.send(std::move(r_c));

    auto end = high_resolution_clock::now();
    
    co_await to_s1.flush();
    co_await to_s2.flush();
    co_await to_c.flush();

    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    stats.sent_bytes = to_s1.bytesSent() + to_s2.bytesSent() + to_c.bytesSent();
}

task<void> server_protocol(Socket& from_e, Socket& from_c, const std::vector<int>& database, int q, Stats& stats) {
    auto start = high_resolution_clock::now();
    int M = database.size();
    std::vector<int> r_s(M), query(M);

    co_await from_e.recv(r_s);
    co_await from_c.recv(query);

    std::vector<int> response(M);
    for (int i = 0; i < M; ++i) {
        response[i] = (query[i] * database[i] + r_s[i]) % q;
    }
    co_await from_c.send(std::move(response));

    auto end = high_resolution_clock::now();
    co_await from_c.flush();

    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    stats.sent_bytes = from_e.bytesSent() + from_c.bytesSent();
    stats.recv_bytes = from_e.bytesReceived() + from_c.bytesReceived();
}

task<void> client_protocol(Socket& from_e, Socket& to_s1, Socket& to_s2, 
                           const std::vector<int>& ind, int q, int M, std::vector<int>& recovered_results, Stats& stats) {
    auto start = high_resolution_clock::now();
    std::vector<int> r_c(M);
    co_await from_e.recv(r_c);

    std::vector<int> v(M, 0);
    for (int idx : ind) if(idx < M) v[idx] = 1;

    std::vector<int> q1 = r_c;
    std::vector<int> q2 = vec_add(r_c, v, q);

    co_await when_all_ready(to_s1.send(std::move(q1)), to_s2.send(std::move(q2)));
    std::vector<int> a1(M), a2(M);
    co_await when_all_ready(to_s1.recv(a1), to_s2.recv(a2));

    for (int idx : ind) {
        if (idx < M) {
            recovered_results[idx] = (a2[idx] - a1[idx] + q) % q;
        }
    }

    auto end = high_resolution_clock::now();

    stats.duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    stats.sent_bytes = from_e.bytesSent() + to_s1.bytesSent() + to_s2.bytesSent();
    stats.recv_bytes = from_e.bytesReceived() + to_s1.bytesReceived() + to_s2.bytesReceived();
}

void run_spir(sgx_enclave_id_t eid, int M, int q, int b, const std::vector<int>& database, const std::vector<int>& ind) {
    Stats eStats{"Enc_Node"}, s1Stats{"Server 1"}, s2Stats{"Server 2"}, cStats{"Client"};
    std::vector<int> recovered_results(M, -1);
    auto e_s1 = LocalAsyncSocket::makePair();
    auto e_s2 = LocalAsyncSocket::makePair();
    auto e_c  = LocalAsyncSocket::makePair();
    auto c_s1 = LocalAsyncSocket::makePair();
    auto c_s2 = LocalAsyncSocket::makePair();

    auto total_start = high_resolution_clock::now();

    sync_wait(when_all_ready(
        enclave_protocol_proxy(eid, e_s1[0], e_s2[0], e_c[0], q, M, eStats),
        server_protocol(e_s1[1], c_s1[0], database, q, s1Stats),
        server_protocol(e_s2[1], c_s2[0], database, q, s2Stats),
        client_protocol(e_c[1], c_s1[1], c_s2[1], ind, q, M, recovered_results, cStats)
    ));

    auto total_end = high_resolution_clock::now();
    double total_time = duration_cast<microseconds>(total_end - total_start).count() / 1000.0;

    // std::cout << "\n[Correctness Check]" << std::endl;
    // bool all_match = true;
    // for (int idx : ind) {
    //     int actual = database[idx];
    //     int recovered = recovered_results[idx];
    //     if (actual != recovered) {
    //         std::cout << "  (!) Mismatch at Index " << idx << ": DB=" << actual << ", Recovered=" << recovered << std::endl;
    //         all_match = false;
    //     } else {
    //         std::cout << "  (+) Match at Index " << idx << ": Value=" << recovered << std::endl;
    //     }
    // }

    // if (all_match) {
    //     std::cout << "  >>> RESULT: SPIR Protocol Verified Successfully." << std::endl;
    // } else {
    //     std::cout << "  >>> RESULT: SPIR Protocol Verification FAILED." << std::endl;
    // }

    std::cout << std::string(85, '=') << std::endl;
    std::cout << "[SPIR Result]" << std::endl;
    std::cout << std::string(85, '-') << std::endl;

    eStats.print();
    s1Stats.print();
    s2Stats.print();
    cStats.print();

    uint64_t total_sent = eStats.sent_bytes + s1Stats.sent_bytes + s2Stats.sent_bytes + cStats.sent_bytes;
    
    std::cout << std::string(85, '-') << std::endl;
    std::cout << "Total Runtime      : " << std::fixed << std::setprecision(3) << total_time << " ms" << std::endl;
    std::cout << "Total Communication: " << std::fixed << std::setprecision(6) << total_sent / (1024.0 * 1024.0) << " MB" << std::endl;
    std::cout << std::string(85, '=') << std::endl;
}