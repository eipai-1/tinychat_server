#pragma once

#include "utils/config.hpp"

#include <mutex>
#include <queue>
#include <memory>
// #include <semaphore.h>
#include <semaphore>  //C++ 20

#include <mysql/jdbc.h>

using Connection = sql::Connection;
using MySQL_Driver = sql::mysql::MySQL_Driver;

namespace tcs {
namespace db {

class SqlConnPool {
public:
    static SqlConnPool* instance();
    Connection* getConn();
    void init();
    void freeConn(Connection* sql);
    void closePool();

private:
    tcs::utils::AppConfig::Database db_cfg_;
    std::mutex mtx_;
    std::unique_ptr<std::counting_semaphore<SEMAPHORE_MAX_VALUE>> smph_;
    int max_conn_;
    std::queue<sql::Connection*> conn_queue_;
    Connection* getSql();

    SqlConnPool();
    ~SqlConnPool();
};
}  // namespace db
}  // namespace tcs