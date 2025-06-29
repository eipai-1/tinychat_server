#include <thread>
#include <iostream>
#include <semaphore>
#include <vector>
#include <chrono>
#include <memory>

#include "utils/config.hpp"
#include "db/sql_conn_RAII.hpp"
#include "db/sql_conn_pool.hpp"
#include "model/auth_models.hpp"

using SqlConnRAII = tcs::db::SqlConnRAII;
using SqlConnPool = tcs::db::SqlConnPool;
using AppConfig = tcs::utils::AppConfig;

void test_sql_conn_pool() {
    AppConfig::get().init("../../doc/config.ini");
    tcs::db::SqlConnPool::instance()->init();

    std::vector<std::thread> threads;
    std::vector<result_type> results;
    std::mutex mtx;

    std::vector<std::string> queries = {
        "SELECT id FROM users",    "SELECT username FROM users",   "SELECT password FROM users",
        "SELECT email FROM users", "SELECT created_at FROM users", "SELECT updated_at FROM users"};

    for (const auto &query : queries) {
        threads.emplace_back([&results, &mtx, query] {
            auto conn = tcs::db::SqlConnRAII();
            // auto result = conn.query(query);
            std::lock_guard<std::mutex> lock(mtx);
            // results.push_back(result);
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

void test_mysql_conn() {
    AppConfig::get().init("../../doc/config.ini");
    SqlConnPool::instance()->init();
    SqlConnRAII connRAII;

    std::unique_ptr<sql::ResultSet> res(
        connRAII.execute_query("SELECT * FROM users WHERE username = ? AND password = ?",
                               std::string("Frank"), std::string("Frank_111")));

    if (res->next()) {
        std::cout << "User ID: " << res->getInt("id") << std::endl;
        std::cout << "Email: " << res->getString("email") << std::endl;
        std::cout << "Created At: " << res->getString("created_at") << std::endl;
    } else {
        std::cout << "No user found with the username 'Frank'." << std::endl;
    }
}

void test_cmake() {
#ifdef PLATFORM_WINDOWS
    std::cout << "This is Windows platform!" << std::endl;
#else
    std::cout << "This is not Windows platform!" << std::endl;
#endif
}

void test_config(int argc, char *argv[]) {
    try {
        tcs::utils::AppConfig::get().init("../../doc/config.ini");
    } catch (const std::exception &e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
    }
}

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

void init() {
    AppConfig::get().init("../../doc/config.ini");

    SqlConnPool::instance()->init();
}

void test_tag_invoke() {}

int main(int argc, char *argv[]) {
    try {
        init();
        test_tag_invoke();
    } catch (std::exception &e) {
        std::cerr << "Excpetion in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}