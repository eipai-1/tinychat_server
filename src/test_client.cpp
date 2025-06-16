#include <semaphore.h>
#include <thread>
#include <iostream>
#include <semaphore>
#include <vector>
#include <chrono>
#include <memory>

#include "utils/config.h"
#include "pool/sql_conn_RAII.hpp"
#include "pool/sql_conn_pool.hpp"

void test_sql_conn_pool() {
    AppConfig config("config.ini");
    SqlConnPool::instance()->init(config.database);

    std::vector<std::thread> threads;
    std::vector<result_type> results;
    std::mutex mtx;

    std::vector<std::string> queries = {
        "SELECT id FROM users",    "SELECT username FROM users",   "SELECT password FROM users",
        "SELECT email FROM users", "SELECT created_at FROM users", "SELECT updated_at FROM users"};

    for (const auto &query : queries) {
        threads.emplace_back([&results, &mtx, query] {
            auto conn = SqlConnRAII(SqlConnPool::instance());
            auto result = conn.query(query);
            std::lock_guard<std::mutex> lock(mtx);
            results.push_back(result);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (auto &r : results) {
        for (auto &m : r) {
            for (auto &[key, value] : m) {
                std::cout << key << ": ";
                if (value.has_value()) {
                    std::cout << value.value() << std::endl;
                } else {
                    std::cout << "NULL" << std::endl;
                }
            }
        }
    }
}

void test_config() {}

void test_std_semaphore() {
    int count = 8;

    std::counting_semaphore<> smph(5);

    std::vector<std::thread> threads;

    for (int i = 0; i < count; i++) {
        threads.emplace_back([i, &smph] {
            std::cout << "Thread " << i << " waiting..." << std::endl;
            smph.acquire();
            std::cout << "Thread " << i << " acquired===" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            smph.release();
            std::cout << "Thread " << i << " released!" << std::endl;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (int i = 0; i < count; i++) {
        threads[i].join();
    }
}

int main() {
    test_sql_conn_pool();
    return 0;
}