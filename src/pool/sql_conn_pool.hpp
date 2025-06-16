#pragma once

#include "utils/config.h"
#include <mysql/mysql.h>
#include <mutex>
#include <queue>
#include <memory>
// #include <semaphore.h>
#include <semaphore>  //C++ 20

class SqlConnPool {
public:
    static SqlConnPool* instance();
    MYSQL* getConn();
    void init(const AppConfig::Database& database_config);
    void freeConn(MYSQL* sql);
    void closePool();

private:
    AppConfig::Database db_cfg_;
    std::mutex mtx_;
    std::unique_ptr<std::counting_semaphore<SEMAPHORE_MAX_VALUE>> smph_;
    int max_conn_;
    std::queue<MYSQL*> conn_queue_;
    MYSQL* getSql();

    SqlConnPool();
    ~SqlConnPool();
};