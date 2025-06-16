#include "sql_conn_pool.hpp"
#include <stdexcept>
#include <memory>
#include <mutex>

SqlConnPool* SqlConnPool::instance() {
    static SqlConnPool pool;
    return &pool;
}

/*
 * 获取连接
 * 会进行健康检查，如果连接不可用则重新初始化连接
 */
MYSQL* SqlConnPool::getConn() {
    if (smph_->try_acquire()) {
        return getSql();
    }
    // 通过日志判断连接池负载
    std::cout << "Sql connect pool is busy..." << std::endl;

    smph_->acquire();
    return getSql();
}

void SqlConnPool::init(const AppConfig::Database& database_config) {
    db_cfg_ = database_config;
    max_conn_ = db_cfg_.sqlconnpool_max_size;
    smph_ = std::make_unique<std::counting_semaphore<SEMAPHORE_MAX_VALUE>>(
        db_cfg_.sqlconnpool_max_size);
    for (int i{}; i < max_conn_; i++) {
        MYSQL* sql = mysql_init(nullptr);
        if (!sql) {
            mysql_close(sql);
            while (!conn_queue_.empty()) {
                mysql_close(conn_queue_.front());
                conn_queue_.pop();
            }
            std::cerr << "Mysql init error!" << std::endl;
            throw std::runtime_error("MySql init error!");
        }

        if (mysql_real_connect(sql, db_cfg_.host.c_str(), db_cfg_.user.c_str(),
                               db_cfg_.passwd.c_str(), db_cfg_.db.c_str(), db_cfg_.port, nullptr,
                               0)) {
            conn_queue_.push(sql);
        } else {
            mysql_close(sql);
            while (!conn_queue_.empty()) {
                mysql_close(conn_queue_.front());
                conn_queue_.pop();
            }
            std::cerr << "Mysql connect error: " << mysql_error(sql) << std::endl;
            throw std::runtime_error("MySql connect error!");
        }
    }
}

void SqlConnPool::freeConn(MYSQL* sql) {
    // 检查连接是否有效
    if (sql) {
        std::lock_guard lock(mtx_);
        conn_queue_.push(sql);
        smph_->release();
    }
}

void SqlConnPool::closePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    while (!conn_queue_.empty()) {
        auto sql = conn_queue_.front();
        mysql_close(sql);
        conn_queue_.pop();
    }
    mysql_library_end();
}

SqlConnPool::~SqlConnPool() { closePool(); }

/*
等待测试--失效连接重新激活
*/
MYSQL* SqlConnPool::getSql() {
    MYSQL* sql = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // 如果队列空了，这通常表示一个逻辑错误，因为信号量保证了有连接可用
        // 但作为健壮性检查，可以加上
        if (conn_queue_.empty()) {
            // 这种情况理论上不该发生，因为 acquire() 成功了
            // 可以选择抛出异常或创建一个新连接
            std::cerr << "CRITICAL: Semaphore acquired but connection queue is empty!" << std::endl;
            // 这里我们选择抛出异常，因为它指示了一个严重的同步问题
            throw std::runtime_error("Connection pool synchronization error.");
        }
        sql = conn_queue_.front();
        conn_queue_.pop();
    }

    if (mysql_ping(sql) == 0) {
        return sql;
    }

    std::cout << "A connection is dead, re-initilizing a new one" << std::endl;
    mysql_close(sql);

    sql = mysql_init(nullptr);
    if (!sql) {
        smph_->release();
        std::cerr << "Re initializing sql failed! Sql conn pool is incompeleted" << std::endl;
        throw std::runtime_error("Re initializing sql failed! Sql conn pool is incompeleted");
    }

    if (mysql_real_connect(sql, db_cfg_.host.c_str(), db_cfg_.user.c_str(), db_cfg_.passwd.c_str(),
                           db_cfg_.db.c_str(), db_cfg_.port, nullptr, 0)) {
        return sql;
    } else {
        mysql_close(sql);
        smph_->release();
        std::cerr << "Re connecting sql failed! Sql conn pool is incompeleted" << std::endl;
        throw std::runtime_error("Re connecting sql failed! Sql conn pool is incompeleted");
    }
}

SqlConnPool::SqlConnPool() : smph_(nullptr) {}